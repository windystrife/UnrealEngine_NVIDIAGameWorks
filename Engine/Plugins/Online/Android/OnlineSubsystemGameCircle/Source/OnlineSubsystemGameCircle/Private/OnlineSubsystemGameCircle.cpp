// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGameCircle.h"
#include "Misc/ConfigCacheIni.h"
#include <jni.h>

FOnlineSubsystemGameCircle::FOnlineSubsystemGameCircle()
	: IdentityInterface(nullptr)
	, LeaderboardsInterface(nullptr)
	, AchievementsInterface(nullptr)
	, StoreInterface(nullptr)
	, AGSCallbackManager(nullptr)
{
}

FOnlineSubsystemGameCircle::FOnlineSubsystemGameCircle(FName InInstanceName)
	: FOnlineSubsystemImpl(GAMECIRCLE_SUBSYSTEM, InInstanceName)
	, IdentityInterface(nullptr)
	, LeaderboardsInterface(nullptr)
	, AchievementsInterface(nullptr)
	, StoreInterface(nullptr)
	, AGSCallbackManager(nullptr)
{
}

IOnlineIdentityPtr FOnlineSubsystemGameCircle::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineStorePtr FOnlineSubsystemGameCircle::GetStoreInterface() const
{
	return StoreInterface;
}

IOnlineSessionPtr FOnlineSubsystemGameCircle::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemGameCircle::GetFriendsInterface() const
{
	return FriendsInterface;
}

IOnlinePartyPtr FOnlineSubsystemGameCircle::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemGameCircle::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemGameCircle::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemGameCircle::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemGameCircle::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemGameCircle::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemGameCircle::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemGameCircle::GetTimeInterface() const
{
	return nullptr;
}

IOnlineTitleFilePtr FOnlineSubsystemGameCircle::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemGameCircle::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemGameCircle::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

static bool WaitForLostFocus = false;
static bool WaitingForLogin = false;

bool FOnlineSubsystemGameCircle::Init() 
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemAndroid::Init"));

	IdentityInterface = MakeShareable(new FOnlineIdentityGameCircle(this));
	LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsGameCircle(this));
	AchievementsInterface = MakeShareable(new FOnlineAchievementsGameCircle(this));
	ExternalUIInterface = MakeShareable(new FOnlineExternalUIGameCircle(this));
	FriendsInterface = MakeShareable(new FOnlineFriendsInterfaceGameCircle(this));
	AGSCallbackManager = MakeShareable(new FOnlineAGSCallbackManager());

	if (IsInAppPurchasingEnabled())
	{
		StoreInterface = MakeShareable(new FOnlineStoreGameCircle(this));
	}

	return true;
}

bool FOnlineSubsystemGameCircle::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	AGSCallbackManager->Tick();

	return true;
}


bool FOnlineSubsystemGameCircle::Shutdown() 
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemAndroid::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
	ensure(Interface.IsUnique()); \
	Interface = NULL; \
	}

	// Destruct the interfaces
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
#undef DESTRUCT_INTERFACE

	return true;
}


FString FOnlineSubsystemGameCircle::GetAppId() const 
{
	//get app id from settings. 
	return TEXT( "AndroidAppIDPlaceHolder" );
}


bool FOnlineSubsystemGameCircle::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	return false;
}

FText FOnlineSubsystemGameCircle::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemGameCircle", "OnlineServiceName", "Amazon GameCircle");
}

bool FOnlineSubsystemGameCircle::IsEnabled(void)
{
	bool bEnabled = true;
	GConfig->GetBool(TEXT("/Script/GameCircleRuntimeSettings.GameCircleRuntimeSettings"), TEXT("bEnableAmazonGameCircleSupport"), bEnabled, GEngineIni);
	return bEnabled;
}

bool FOnlineSubsystemGameCircle::IsInAppPurchasingEnabled()
{
	bool bEnabledIAP = false;
	GConfig->GetBool(TEXT("/Script/GameCircleRuntimeSettings.GameCircleRuntimeSettings"), TEXT("bSupportsInAppPurchasing"), bEnabledIAP, GEngineIni);
	return bEnabledIAP;
}


std::string FOnlineSubsystemGameCircle::ConvertFStringToStdString(const FString& InString)
{
	int32 SrcLen  = InString.Len() + 1;
	int32 DestLen = FPlatformString::ConvertedLength<ANSICHAR>(*InString, SrcLen);
	TArray<ANSICHAR> Converted;
	Converted.AddUninitialized(DestLen);
	
	FPlatformString::Convert(Converted.GetData(), DestLen, *InString, SrcLen);

	return std::string(Converted.GetData());
}


JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeOnAmazonGamesInitCallback(JNIEnv* jenv, jobject thiz, jboolean bServiceIsReady)
{
	FOnlineSubsystemGameCircle * Subsystem = (FOnlineSubsystemGameCircle *)(FOnlineSubsystemGameCircle::Get());

	if(bServiceIsReady == JNI_TRUE && Subsystem)
	{
		checkf(Subsystem->GetIdentityGameCircle().IsValid(), TEXT("Is your OnlineSubsystem set to GameCircle in AndroidEngine.ini?"));
		Subsystem->GetIdentityGameCircle()->RequestLocalPlayerInfo();
		Subsystem->GetExternalUIGameCircle()->GameActivityOnResume();
	}
	else
	{
		UE_LOG(LogOnline, Error, TEXT("Skipped request for local player info. ServiceIsReady = %s"), (bServiceIsReady == JNI_TRUE) ? TEXT("TRUE") : TEXT("FALSE"));
	}
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeGameCircleOnResume(JNIEnv* jenv, jobject thiz)
{
	FOnlineSubsystemGameCircle * Subsystem = (FOnlineSubsystemGameCircle *)(FOnlineSubsystemGameCircle::Get());

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Java_com_epicgames_ue4_GameActivity_nativeGameCircleOnResume"));

	if(Subsystem && Subsystem->GetExternalUIGameCircle().IsValid())
	{
		Subsystem->GetExternalUIGameCircle()->GameActivityOnResume();
	}
}
