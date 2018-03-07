// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtilsModule.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online.h"
#include "EngineGlobals.h"

#ifdef ONLINESUBSYSTEMUTILS_API

/** @return an initialized audio component specifically for use with VoIP */
ONLINESUBSYSTEMUTILS_API class UAudioComponent* CreateVoiceAudioComponent(uint32 SampleRate, int32 NumChannels);

/** @return the world associated with a named online subsystem instance */
ONLINESUBSYSTEMUTILS_API UWorld* GetWorldForOnline(FName InstanceName);

/**
 * Try to retrieve the active listen port for a server session
 *
 * @param InstanceName online subsystem instance to query
 *
 * @return the port number currently associated with the GAME net driver
 */
ONLINESUBSYSTEMUTILS_API int32 GetPortFromNetDriver(FName InstanceName);

#endif

/**
 * Interface class for various online utility functions
 */
class IOnlineSubsystemUtils
{
protected:
	/** Hidden on purpose */
	IOnlineSubsystemUtils() {}

public:

	virtual ~IOnlineSubsystemUtils() {}

	/**
	 * Gets an FName that uniquely identifies an instance of OSS
	 *
	 * @param WorldContext the worldcontext associated with a particular subsystem
	 * @param Subsystem the name of the subsystem
	 * @return an FName of format Subsystem:Context_Id in PlayInEditor or Subsystem everywhere else
	 */
	virtual FName GetOnlineIdentifier(const FWorldContext& WorldContext, const FName Subsystem = NAME_None) = 0;

	/**
	 * Gets an FName that uniquely identifies an instance of OSS
	 *
	 * @param World the world to use for context
	 * @param Subsystem the name of the subsystem
	 * @return an FName of format Subsystem:Context_Id in PlayInEditor or Subsystem everywhere else
	 */
	virtual FName GetOnlineIdentifier(UWorld* World, const FName Subsystem = NAME_None) = 0;

#if WITH_EDITOR
	/**
	 * Play in Editor settings
	 */

	/** @return true if the default platform supports logging in for Play In Editor (PIE) */
	virtual bool SupportsOnlinePIE() const = 0;
	/** Enable/Disable online PIE at runtime */
	virtual void SetShouldTryOnlinePIE(bool bShouldTry) = 0;
	/** @return true if the user has enabled logging in for Play In Editor (PIE) */
	virtual bool IsOnlinePIEEnabled() const = 0;
	/** @return the number of logins the user has setup for Play In Editor (PIE) */
	virtual int32 GetNumPIELogins() const = 0;
	/** @return the array of valid credentials the user has setup for Play In Editor (PIE) */
	virtual void GetPIELogins(TArray<FOnlineAccountCredentials>& Logins) = 0;
#endif // WITH_EDITOR
};

/** Macro to handle the boilerplate of accessing the proper online subsystem and getting the requested interface (UWorld version) */
#define IMPLEMENT_GET_INTERFACE(InterfaceType) \
static IOnline##InterfaceType##Ptr Get##InterfaceType##Interface(class UWorld* World, const FName SubsystemName = NAME_None) \
{ \
	IOnlineSubsystem* OSS = Online::GetSubsystem(World, SubsystemName); \
	return (OSS == NULL) ? NULL : OSS->Get##InterfaceType##Interface(); \
}

namespace Online
{
	/** @return the single instance of the online subsystem utils interface */
	static IOnlineSubsystemUtils* GetUtils()
	{
		static const FName OnlineSubsystemModuleName = TEXT("OnlineSubsystemUtils");
		FOnlineSubsystemUtilsModule* OSSUtilsModule = FModuleManager::GetModulePtr<FOnlineSubsystemUtilsModule>(OnlineSubsystemModuleName);
		if (OSSUtilsModule != nullptr)
		{
			return OSSUtilsModule->GetUtils();
		}

		return nullptr;
	}

	/** 
	 * Get the online subsystem for a given service
	 *
	 * @param World the world to use for context
	 * @param SubsystemName - Name of the requested online service
	 *
	 * @return pointer to the appropriate online subsystem
	 */
	static IOnlineSubsystem* GetSubsystem(UWorld* World, const FName& SubsystemName = NAME_None)
	{
#if UE_EDITOR // at present, multiple worlds are only possible in the editor
		FName Identifier = SubsystemName; 
		if (World != NULL)
		{
			IOnlineSubsystemUtils* Utils = GetUtils();
			Identifier = Utils->GetOnlineIdentifier(World, SubsystemName);
		}

		return IOnlineSubsystem::Get(Identifier); 
#else
		return IOnlineSubsystem::Get(SubsystemName); 
#endif
	}

	/** 
	 * Determine if the subsystem for a given interface is already loaded
	 *
	 * @param World the world to use for context
	 * @param SubsystemName name of the requested online service
	 *
	 * @return true if module for the subsystem is loaded
	 */
	static bool IsLoaded(UWorld* World, const FName& SubsystemName = NAME_None)
	{
#if UE_EDITOR // at present, multiple worlds are only possible in the editor
		FName Identifier = SubsystemName;
		if (World != NULL)
		{
			FWorldContext& CurrentContext = GEngine->GetWorldContextFromWorldChecked(World);
			if (CurrentContext.WorldType == EWorldType::PIE)
			{
				Identifier = FName(*FString::Printf(TEXT("%s:%s"), SubsystemName != NAME_None ? *SubsystemName.ToString() : TEXT(""), *CurrentContext.ContextHandle.ToString()));
			}
		}

		return IOnlineSubsystem::IsLoaded(SubsystemName);
#else
		return IOnlineSubsystem::IsLoaded(SubsystemName);
#endif
	}

	/** Reimplement all the interfaces of Online.h with support for UWorld accessors */
	IMPLEMENT_GET_INTERFACE(Session);
	IMPLEMENT_GET_INTERFACE(Party);
	IMPLEMENT_GET_INTERFACE(Chat);
	IMPLEMENT_GET_INTERFACE(Friends);
	IMPLEMENT_GET_INTERFACE(User);
	IMPLEMENT_GET_INTERFACE(SharedCloud);
	IMPLEMENT_GET_INTERFACE(UserCloud);
	IMPLEMENT_GET_INTERFACE(Voice);
	IMPLEMENT_GET_INTERFACE(ExternalUI);
	IMPLEMENT_GET_INTERFACE(Time);
	IMPLEMENT_GET_INTERFACE(Identity);
	IMPLEMENT_GET_INTERFACE(TitleFile);
	IMPLEMENT_GET_INTERFACE(Entitlements);
	IMPLEMENT_GET_INTERFACE(Leaderboards);
	IMPLEMENT_GET_INTERFACE(Achievements);
	IMPLEMENT_GET_INTERFACE(Presence);
}

#undef IMPLEMENT_GET_INTERFACE
