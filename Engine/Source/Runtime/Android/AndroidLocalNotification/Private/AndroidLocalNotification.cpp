// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	AndroidLocalNotification.cpp: Unreal AndroidLocalNotification service interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "../Public/AndroidLocalNotification.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"

DEFINE_LOG_CATEGORY(LogAndroidLocalNotification);

class FAndroidLocalNotificationModule : public ILocalNotificationModule
{
public:
	/** Creates a new instance of the service implemented by the module. */
	virtual ILocalNotificationService* GetLocalNotificationService() override
	{
		static ILocalNotificationService*	oneTrueLocalNotificationService = nullptr;
		
		if(oneTrueLocalNotificationService == nullptr)
		{
			oneTrueLocalNotificationService = new FAndroidLocalNotificationService;
		}
		
		return oneTrueLocalNotificationService;
	}
};


#if PLATFORM_ANDROID
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeAppOpenedWithLocalNotification(JNIEnv* jenv, jobject thiz, jstring jactivationEvent, int32 jFireDate)
{

	FString ActivationEvent;
	const char* ActivationEventChars = jenv->GetStringUTFChars(jactivationEvent, 0);
	ActivationEvent = FString(UTF8_TO_TCHAR(ActivationEventChars));
	jenv->ReleaseStringUTFChars(jactivationEvent, ActivationEventChars);
	
	int32 FireDate = (int32)jFireDate;

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessAppOpenedWithLocalNotification"), STAT_FSimpleDelegateGraphTask_ProcessAppOpenedWithLocalNotification, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]() 
		{
			ILocalNotificationService* AndroidLocalNotificationService = NULL;

			FString ModuleName = "AndroidLocalNotification";
			// load the module by name
			ILocalNotificationModule* module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName);
			// did the module exist?
			if (module != nullptr)
			{
				AndroidLocalNotificationService = module->GetLocalNotificationService();
				AndroidLocalNotificationService->SetLaunchNotification(ActivationEvent, FireDate);
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessAppOpenedWithLocalNotification),
		nullptr,
		ENamedThreads::GameThread
		);
}
#endif


IMPLEMENT_MODULE(FAndroidLocalNotificationModule, AndroidLocalNotification);

/*------------------------------------------------------------------------------------
	FAndroidLocalNotification
 ------------------------------------------------------------------------------------*/
FAndroidLocalNotificationService::FAndroidLocalNotificationService(){
	AppLaunchedWithNotification = false;
	LaunchNotificationActivationEvent = "None";
	LaunchNotificationFireDate = 0;
}

void FAndroidLocalNotificationService::ClearAllLocalNotifications()
{
	#if PLATFORM_ANDROID
		extern void AndroidThunkCpp_ClearAllLocalNotifications();
		AndroidThunkCpp_ClearAllLocalNotifications();
	#endif
}

void FAndroidLocalNotificationService::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
	#if PLATFORM_ANDROID
		extern void AndroidThunkCpp_ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent);
		AndroidThunkCpp_ScheduleLocalNotificationAtTime(FireDateTime , LocalTime, Title, Body, Action, ActivationEvent);
	#endif
}

void FAndroidLocalNotificationService::ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool LocalTime, const FString& ActivationEvent)
{
	// Do nothing...
}

void FAndroidLocalNotificationService::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	#if PLATFORM_ANDROID
		extern void AndroidThunkCpp_GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate);
		AndroidThunkCpp_GetLaunchNotification(NotificationLaunchedApp, ActivationEvent, FireDate);
	#endif
}

void FAndroidLocalNotificationService::SetLaunchNotification(FString const& ActivationEvent, int32 FireDate)
{
	// Don't do anything here since this is taken care of in java land
}

void FAndroidLocalNotificationService::CancelLocalNotification(const FString& ActivationEvent)
{
	// TODO
}
