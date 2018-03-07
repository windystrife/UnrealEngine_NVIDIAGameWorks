// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/CoreOnline.h"
#include "Net/OnlineEngineInterface.h"
#include "OnlineEngineInterfaceImpl.generated.h"

/** If OnlineEngineInterface.h doesn't define this, it isn't available. */
#ifndef OSS_DEDICATED_SERVER_VOICECHAT
#define OSS_DEDICATED_SERVER_VOICECHAT 0
#endif

class Error;
class FVoicePacket;
struct FWorldContext;

UCLASS(config = Engine)
class ONLINESUBSYSTEMUTILS_API UOnlineEngineInterfaceImpl : public UOnlineEngineInterface
{
	GENERATED_UCLASS_BODY()

	/**
	 * Subsystem
	 */
public:

	virtual bool IsLoaded(FName OnlineIdentifier) override;
	virtual FName GetOnlineIdentifier(FWorldContext& WorldContext) override;
	virtual bool DoesInstanceExist(FName OnlineIdentifier) override;
	virtual void ShutdownOnlineSubsystem(FName OnlineIdentifier) override;
	virtual void DestroyOnlineSubsystem(FName OnlineIdentifier) override;
#if OSS_DEDICATED_SERVER_VOICECHAT
	virtual FName GetDefaultOnlineSubsystemName() const override;
#endif

private:

	/** Allow the subsystem used for voice functions to be overridden, in case it needs to be different than the default subsystem. May be useful on console platforms. */
	UPROPERTY(config)
	FName VoiceSubsystemNameOverride;

	/** Cache the name of the default subsystem, returned by GetDefaultOnlineSubsystemName(). */
	FName DefaultSubsystemName;

	FName GetOnlineIdentifier(UWorld* World);

	/** Returns the name of a corresponding dedicated server subsystem for the given subsystem, or NAME_None if such a system doesn't exist. */
	FName GetDedicatedServerSubsystemNameForSubsystem(const FName Subsystem) const;

	/**
	 * Identity
	 */
public:

	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(UWorld* World, int32 LocalUserNum) override;

	virtual FString GetPlayerNickname(UWorld* World, const FUniqueNetId& UniqueId) override;
	virtual bool GetPlayerPlatformNickname(UWorld* World, int32 LocalUserNum, FString& OutNickname) override;

	virtual bool AutoLogin(UWorld* World, int32 LocalUserNum, const FOnlineAutoLoginComplete& InCompletionDelegate) override;
	virtual bool IsLoggedIn(UWorld* World, int32 LocalUserNum) override;

private:

	FDelegateHandle OnLoginCompleteDelegateHandle;
	void OnAutoLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FName OnlineIdentifier, FOnlineAutoLoginComplete InCompletionDelegate);

	/**
	 * Session
	 */
public:

	virtual void StartSession(UWorld* World, FName SessionName, FOnlineSessionStartComplete& InCompletionDelegate) override;
	virtual void EndSession(UWorld* World, FName SessionName, FOnlineSessionEndComplete& InCompletionDelegate) override;
	virtual bool DoesSessionExist(UWorld* World, FName SessionName) override;

	virtual bool GetSessionJoinability(UWorld* World, FName SessionName, FJoinabilitySettings& OutSettings) override;
	virtual void UpdateSessionJoinability(UWorld* World, FName SessionName, bool bPublicSearchable, bool bAllowInvites, bool bJoinViaPresence, bool bJoinViaPresenceFriendsOnly) override;

	virtual void RegisterPlayer(UWorld* World, FName SessionName, const FUniqueNetId& UniqueId, bool bWasInvited) override;
	virtual void UnregisterPlayer(UWorld* World, FName SessionName, const FUniqueNetId& UniqueId) override;

	virtual bool GetResolvedConnectString(UWorld* World, FName SessionName, FString& URL) override;

private:

	/** Mapping of delegate handles for each online StartSession() call while in flight */
	TMap<FName, FDelegateHandle> OnStartSessionCompleteDelegateHandles;
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful, FName OnlineIdentifier, FOnlineSessionStartComplete CompletionDelegate);

	/** Mapping of delegate handles for each online EndSession() call while in flight */
	TMap<FName, FDelegateHandle> OnEndSessionCompleteDelegateHandles;
	void OnEndSessionComplete(FName SessionName, bool bWasSuccessful, FName OnlineIdentifier, FOnlineSessionEndComplete CompletionDelegate);

	/**
	 * Voice
	 */
public:

	virtual TSharedPtr<FVoicePacket> GetLocalPacket(UWorld* World, uint8 LocalUserNum) override;
#if OSS_DEDICATED_SERVER_VOICECHAT
	virtual TSharedPtr<FVoicePacket> SerializeRemotePacket(UWorld* World, const UNetConnection* const RemoteConnection, FArchive& Ar) override;
#else
	virtual TSharedPtr<FVoicePacket> SerializeRemotePacket(UWorld* World, FArchive& Ar) override;
#endif

	virtual void StartNetworkedVoice(UWorld* World, uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(UWorld* World, uint8 LocalUserNum) override;
	virtual void ClearVoicePackets(UWorld* World) override;

	virtual bool MuteRemoteTalker(UWorld* World, uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(UWorld* World, uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;

	virtual int32 GetNumLocalTalkers(UWorld* World) override;

	/**
	 * External UI
	 */
public:

	virtual void ShowLeaderboardUI(UWorld* World, const FString& CategoryName) override;
	virtual void ShowAchievementsUI(UWorld* World, int32 LocalUserNum) override;
	virtual void BindToExternalUIOpening(const FOnlineExternalUIChanged& Delegate) override;
#ifdef OSS_ADDED_SHOW_WEB
	virtual void ShowWebURL(const FString& CurrentURL, const UOnlineEngineInterface::FShowWebUrlParams& ShowParams, const FOnlineShowWebUrlClosed& CompletionDelegate) override;
	virtual bool CloseWebURL() override;
#endif

private:

	void OnExternalUIChange(bool bInIsOpening, FOnlineExternalUIChanged Delegate);

	/**
	 * Debug
	 */
public:

	virtual void DumpSessionState(UWorld* World) override;
	virtual void DumpPartyState(UWorld* World) override;
	virtual void DumpVoiceState(UWorld* World) override;
	virtual void DumpChatState(UWorld* World) override;

#if WITH_EDITOR
	/**
	 * PIE Utilities
	 */
public:

	virtual bool SupportsOnlinePIE() override;
	virtual void SetShouldTryOnlinePIE(bool bShouldTry) override;
	virtual int32 GetNumPIELogins() override;
	virtual void SetForceDedicated(FName OnlineIdentifier, bool bForce) override;
	virtual void LoginPIEInstance(FName OnlineIdentifier, int32 LocalUserNum, int32 PIELoginNum, FOnPIELoginComplete& CompletionDelegate) override;
#endif

private:

	/** Mapping of delegate handles for each online Login() call while in flight */
	TMap<FName, FDelegateHandle> OnLoginPIECompleteDelegateHandlesForPIEInstances;
	void OnPIELoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, FName OnlineIdentifier, FOnlineAutoLoginComplete InCompletionDelegate);

};

