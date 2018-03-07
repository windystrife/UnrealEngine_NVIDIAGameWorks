// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "OnlineSubsystem.h"
#include "Containers/Queue.h"
#include "OnlineSubsystemPackage.h"

struct FOnlineError;

DECLARE_DELEGATE(FNextTickDelegate);

namespace OSSConsoleVariables
{
	extern ONLINESUBSYSTEM_API TAutoConsoleVariable<int32> CVarVoiceLoopback;
}

/**
 *	FOnlineSubsystemImpl - common functionality to share across online platforms, not intended for direct use
 */
class ONLINESUBSYSTEM_API FOnlineSubsystemImpl 
	: public IOnlineSubsystem
{
private:

	/**
	 * Exec function handling for Exec() call
	 */
	bool HandleFriendExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleSessionExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandlePurchaseExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
	
	/** Delegate fired when exec cheat related to receipts completes */
	void OnQueryReceiptsComplete(const FOnlineError& Result, TSharedPtr<const FUniqueNetId> UserId);
	
	/** Dump purchase receipts for a given user id */
	void DumpReceipts(const FUniqueNetId& UserId);

protected:

	/** Hidden on purpose */
	FOnlineSubsystemImpl();
	FOnlineSubsystemImpl(FName InSubsystemName, FName InInstanceName);

	/** Name of the subsystem @see OnlineSubsystemNames.h */
	FName SubsystemName;
	/** Instance name (disambiguates PIE instances for example) */
	FName InstanceName;

	/** Whether or not the online subsystem is in forced dedicated server mode */
	bool bForceDedicated;

	/** Holds all currently named interfaces */
	mutable class UNamedInterfaces* NamedInterfaces;

	/** Load in any named interfaces specified by the ini configuration */
	void InitNamedInterfaces();

	/** Delegate fired when named interfaces are cleaned up at exit */
	void OnNamedInterfaceCleanup();

	/** Queue to hold callbacks scheduled for next tick using ExecuteNextTick */
	TQueue<FNextTickDelegate, EQueueMode::Mpsc> NextTickQueue;

	/** Buffer to hold callbacks for the current tick (so it's safe to call ExecuteNextTick within a tick callback) */
	TArray<FNextTickDelegate> CurrentTickBuffer;

	/** Start Ticker */
	void StartTicker();

	/** Stop Ticker */
	void StopTicker();
	
	/** Delegate for callbacks to Tick */
	FDelegateHandle TickHandle;

public:
	
	virtual ~FOnlineSubsystemImpl();

	// IOnlineSubsystem
	virtual void PreUnload() override;
	virtual bool Shutdown() override;
	virtual bool IsServer() const override;
	virtual bool IsDedicated() const override{ return bForceDedicated || IsRunningDedicatedServer(); }
	virtual void SetForceDedicated(bool bForce) override { bForceDedicated = bForce; }
	virtual class UObject* GetNamedInterface(FName InterfaceName) override;
	virtual void SetNamedInterface(FName InterfaceName, class UObject* NewInterface) override;
	virtual bool IsLocalPlayer(const FUniqueNetId& UniqueId) const override;
	virtual void SetUsingMultiplayerFeatures(const FUniqueNetId& UniqueId, bool bUsingMP) override {};
	virtual EOnlineEnvironment::Type GetOnlineEnvironment() const override { return EOnlineEnvironment::Unknown; }
	virtual IMessageSanitizerPtr GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FName GetSubsystemName() const override { return SubsystemName; }
	virtual FName GetInstanceName() const override { return InstanceName; }

	/**
	 * Tick function
	 *
	 * @param DeltaTime	time passed since the last call.
	 * @return true if should continue ticking
	 */
	virtual bool Tick(float DeltaTime);

	/**
	 * Modify a response string so that it can be logged cleanly
	 *
	 * @param ResponseStr - The JSONObject string we want to sanitize
	 * @param RedactFields - The fields we want to specifically omit (optional, only supports EJson::String), if nothing specified everything is redacted
	 * @return the modified version of the response string
	 */
	FString FilterResponseStr(const FString& ResponseStr, const TArray<FString>& RedactFields = TArray<FString>());

	/**
	 * Queue a delegate to be executed on the next tick
	 */
	void ExecuteDelegateNextTick(const FNextTickDelegate& Callback);

	/**
	 * Templated helper for calling ExecuteDelegateNextTick with a lambda function
	 */
	template<typename LAMBDA_TYPE>
	FORCEINLINE void ExecuteNextTick(LAMBDA_TYPE&& Callback)
	{
		ExecuteDelegateNextTick(FNextTickDelegate::CreateLambda(Forward<LAMBDA_TYPE>(Callback)));
	}

	/** Name given to default OSS instances (disambiguates for PIE) */
	static const FName DefaultInstanceName;
};

