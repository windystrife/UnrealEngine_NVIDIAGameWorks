// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintPlatformLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "LocalNotification.h"
#include "EngineLogs.h"

void UPlatformGameInstance::PostInitProperties()

{
    Super::PostInitProperties();

    FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillDeactivateDelegate_Handler);
    FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationHasReactivatedDelegate_Handler);
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillEnterBackgroundDelegate_Handler);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationHasEnteredForegroundDelegate_Handler);
    FCoreDelegates::ApplicationWillTerminateDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationWillTerminateDelegate_Handler);
    FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationRegisteredForRemoteNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationRegisteredForUserNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationFailedToRegisterForRemoteNotificationsDelegate_Handler);
    FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedRemoteNotificationDelegate_Handler);
    FCoreDelegates::ApplicationReceivedLocalNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedLocalNotificationDelegate_Handler);
    FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.AddUObject(this, &UPlatformGameInstance::ApplicationReceivedScreenOrientationChangedNotificationDelegate_Handler);
}

void UPlatformGameInstance::BeginDestroy()

{
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillTerminateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedLocalNotificationDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.RemoveAll(this);

    Super::BeginDestroy();
}

void UPlatformGameInstance::ApplicationReceivedScreenOrientationChangedNotificationDelegate_Handler(int32 inScreenOrientation)
{
	ApplicationReceivedScreenOrientationChangedNotificationDelegate.Broadcast((EScreenOrientation::Type)inScreenOrientation);
}

void UPlatformGameInstance::ApplicationReceivedRemoteNotificationDelegate_Handler(FString inFString, int32 inAppState)
{
	ApplicationReceivedRemoteNotificationDelegate.Broadcast(inFString, (EApplicationState::Type)inAppState);
}

void UPlatformGameInstance::ApplicationReceivedLocalNotificationDelegate_Handler(FString inFString, int32 inInt, int32 inAppState)
{
	ApplicationReceivedLocalNotificationDelegate.Broadcast(inFString, inInt, (EApplicationState::Type)inAppState);
}


//////////////////////////////////////////////////////////////////////////
// UBlueprintPlatformLibrary

UBlueprintPlatformLibrary::UBlueprintPlatformLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (platformService == nullptr)
	{
		FString ModuleName;
		GConfig->GetString(TEXT("LocalNotification"), TEXT("DefaultPlatformService"), ModuleName, GEngineIni);

		if (ModuleName.Len() > 0)
		{
			// load the module by name from the .ini
			if (ILocalNotificationModule* Module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName))
			{
				platformService = Module->GetLocalNotificationService();
			}
		}
	}
}

void UBlueprintPlatformLibrary::ClearAllLocalNotifications()
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("ClearAllLocalNotifications(): No local notification service"));
		return;
	}
	
	platformService->ClearAllLocalNotifications();
}

void UBlueprintPlatformLibrary::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool inLocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("ScheduleLocalNotificationAtTime(): No local notification service"));
		return;
	}

	UE_LOG(LogBlueprintUserMessages, Log, TEXT("Scheduling notification %s at %d/%d/%d %d:%d:%d %s"), *(Title.ToString()), FireDateTime.GetMonth(), FireDateTime.GetDay(), FireDateTime.GetYear(), FireDateTime.GetHour(), FireDateTime.GetMinute(), FireDateTime.GetSecond(), inLocalTime ? TEXT("Local") : TEXT("UTC"));
	
	platformService->ScheduleLocalNotificationAtTime(FireDateTime, inLocalTime, Title, Body, Action, ActivationEvent);
}
       
void UBlueprintPlatformLibrary::ScheduleLocalNotificationFromNow(int32 inSecondsFromNow, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
	FDateTime TargetTime = FDateTime::Now();
	TargetTime += FTimespan::FromSeconds(inSecondsFromNow);

	ScheduleLocalNotificationAtTime(TargetTime, true, Title, Body, Action, ActivationEvent);
}

void UBlueprintPlatformLibrary::ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool inLocalTime, const FString& ActivationEvent)
{
	if (platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("ScheduleLocalNotificationBadgeAtTime(): No local notification service"));
		return;
	}

	UE_LOG(LogBlueprintUserMessages, Log, TEXT("Scheduling notification badge %s at %d/%d/%d %d:%d:%d %s"), *ActivationEvent, FireDateTime.GetMonth(), FireDateTime.GetDay(), FireDateTime.GetYear(), FireDateTime.GetHour(), FireDateTime.GetMinute(), FireDateTime.GetSecond(), inLocalTime ? TEXT("Local") : TEXT("UTC"));

	platformService->ScheduleLocalNotificationBadgeAtTime(FireDateTime, inLocalTime, ActivationEvent);
}

void UBlueprintPlatformLibrary::ScheduleLocalNotificationBadgeFromNow(int32 inSecondsFromNow, const FString& ActivationEvent)
{
	FDateTime TargetTime = FDateTime::Now();
	TargetTime += FTimespan::FromSeconds(inSecondsFromNow);

	ScheduleLocalNotificationBadgeAtTime(TargetTime, true, ActivationEvent);
}

UFUNCTION(BlueprintCallable, Category="Platform|LocalNotification")
void UBlueprintPlatformLibrary::CancelLocalNotification(const FString& ActivationEvent)
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("CancelLocalNotification(): No local notification service"));
		return;
	}

	UE_LOG(LogBlueprintUserMessages, Log, TEXT("Canceling notification %s"), *ActivationEvent);
	
	platformService->CancelLocalNotification(ActivationEvent);
}

void UBlueprintPlatformLibrary::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	if(platformService == nullptr)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("GetLaunchNotification(): No local notification service"));
		return;
	}
	
	platformService->GetLaunchNotification(NotificationLaunchedApp, ActivationEvent, FireDate);
}

ILocalNotificationService* UBlueprintPlatformLibrary::platformService = nullptr;
