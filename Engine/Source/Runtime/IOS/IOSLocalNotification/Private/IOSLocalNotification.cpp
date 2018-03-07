// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	IOSLocalNotification.cpp: Unreal IOSLocalNotification service interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSLocalNotification.h"

#include "IOSApplication.h"
#include "IOSAppDelegate.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogIOSLocalNotification);

class FIOSLocalNotificationModule : public ILocalNotificationModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual ILocalNotificationService* GetLocalNotificationService() override
	{
		static ILocalNotificationService*	oneTrueLocalNotificationService = nullptr;
		
		if(oneTrueLocalNotificationService == nullptr)
		{
			oneTrueLocalNotificationService = new FIOSLocalNotificationService;
		}
		
		return oneTrueLocalNotificationService;
	}

#if !PLATFORM_TVOS
	static UILocalNotification* CreateLocalNotification(const FDateTime& FireDateTime, bool bLocalTime, const FString& ActivationEvent)
	{
		UIApplication* application = [UIApplication sharedApplication];

		NSCalendar *calendar = [NSCalendar autoupdatingCurrentCalendar];
		NSDateComponents *dateComps = [[NSDateComponents alloc] init];
		[dateComps setDay : FireDateTime.GetDay()];
		[dateComps setMonth : FireDateTime.GetMonth()];
		[dateComps setYear : FireDateTime.GetYear()];
		[dateComps setHour : FireDateTime.GetHour()];
		[dateComps setMinute : FireDateTime.GetMinute()];
		[dateComps setSecond : FireDateTime.GetSecond()];
		NSDate *itemDate = [calendar dateFromComponents : dateComps];

		UILocalNotification *localNotif = [[UILocalNotification alloc] init];
		if (localNotif != nil)
		{
			localNotif.fireDate = itemDate;
			if (bLocalTime)
			{
				localNotif.timeZone = [NSTimeZone defaultTimeZone];
			}
			else
			{
				localNotif.timeZone = nil;
			}

			NSString* activateEventNSString = [NSString stringWithFString:ActivationEvent];
			if (activateEventNSString != nil)
			{
				NSDictionary* infoDict = [NSDictionary dictionaryWithObject:activateEventNSString forKey:@"ActivationEvent"];
				if (infoDict != nil)
				{
					localNotif.userInfo = infoDict;
				}
			}
		}
		return localNotif;
	}
#endif
};

IMPLEMENT_MODULE(FIOSLocalNotificationModule, IOSLocalNotification);

/*------------------------------------------------------------------------------------
	FIOSLocalNotification
 ------------------------------------------------------------------------------------*/
FIOSLocalNotificationService::FIOSLocalNotificationService()
{
	AppLaunchedWithNotification = false;
	LaunchNotificationFireDate = 0;
}

void FIOSLocalNotificationService::ClearAllLocalNotifications()
{
#if !PLATFORM_TVOS
	UIApplication* application = [UIApplication sharedApplication];
	
	[application cancelAllLocalNotifications];
#endif
}

void FIOSLocalNotificationService::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
#if !PLATFORM_TVOS
	UILocalNotification *localNotif = FIOSLocalNotificationModule::CreateLocalNotification(FireDateTime, LocalTime, ActivationEvent);
	if (localNotif == nil)
		return;

	NSString*	alertBody = [NSString stringWithFString : Body.ToString()];
	if (alertBody != nil)
	{
		localNotif.alertBody = alertBody;
	}

	NSString*	alertAction = [NSString stringWithFString:Action.ToString()];
	if(alertAction != nil)
	{
		localNotif.alertAction = alertAction;
	}
	
	if([IOSAppDelegate GetDelegate].OSVersion >= 8.2f)
	{
		NSString*	alertTitle = [NSString stringWithFString:Title.ToString()];
		if(alertTitle != nil)
		{
			localNotif.alertTitle = alertTitle;
		}
	}
	
	localNotif.soundName = UILocalNotificationDefaultSoundName;
	localNotif.applicationIconBadgeNumber = 1;

	[[UIApplication sharedApplication] scheduleLocalNotification:localNotif];
#endif
}

void FIOSLocalNotificationService::ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool LocalTime, const FString& ActivationEvent)
{
#if !PLATFORM_TVOS
	UILocalNotification *localNotif = FIOSLocalNotificationModule::CreateLocalNotification(FireDateTime, LocalTime, ActivationEvent);
	if (localNotif == nil)
		return;

	// As per Apple documentation, a nil 'alertBody' results in 'no alert'
	// https://developer.apple.com/reference/uikit/uilocalnotification/1616646-alertbody?language=objc
	localNotif.alertBody = nil;
	localNotif.applicationIconBadgeNumber = 1;

	[[UIApplication sharedApplication] scheduleLocalNotification:localNotif];
#endif
}

void FIOSLocalNotificationService::CancelLocalNotification(const FString& ActivationEvent)
{
	// TODO
}

void FIOSLocalNotificationService::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	NotificationLaunchedApp = AppLaunchedWithNotification;
	ActivationEvent = LaunchNotificationActivationEvent;
	FireDate = LaunchNotificationFireDate;
}

void FIOSLocalNotificationService::SetLaunchNotification(FString const& ActivationEvent, int32 FireDate)
{
	AppLaunchedWithNotification = true;
	LaunchNotificationActivationEvent = ActivationEvent;
	LaunchNotificationFireDate = FireDate;
}
