// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "TickableEditorObject.h"
#include "Templates/ScopedPointer.h"
#include "DistanceFieldAtlas.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UniquePtr.h"

/** Notification class for asynchronous distance field building. */
class FDistanceFieldBuildNotificationImpl
	: public FTickableEditorObject
{

public:

	FDistanceFieldBuildNotificationImpl()
	{
		LastEnableTime = 0;
	}

	/** Starts the notification. */
	void DistanceFieldBuildStarted();

	/** Ends the notification. */
	void DistanceFieldBuildFinished();

protected:
	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override
	{
		return true;
	}
	virtual TStatId GetStatId() const override;

private:

	/** Tracks the last time the notification was started, used to avoid spamming. */
	double LastEnableTime;

	/** In progress message */
	TWeakPtr<SNotificationItem> DistanceFieldNotificationPtr;
};

/** Global notification object. */
TUniquePtr<FDistanceFieldBuildNotificationImpl> GDistanceFieldBuildNotification;

void SetupDistanceFieldBuildNotification()
{
	// Create explicitly to avoid relying on static initialization order
	GDistanceFieldBuildNotification = MakeUnique<FDistanceFieldBuildNotificationImpl>();
}

void TearDownDistanceFieldBuildNotification()
{
	GDistanceFieldBuildNotification = nullptr;
}

void FDistanceFieldBuildNotificationImpl::DistanceFieldBuildStarted()
{
	LastEnableTime = FPlatformTime::Seconds();

	// Starting a new request! Notify the UI.
	if (DistanceFieldNotificationPtr.IsValid())
	{
		DistanceFieldNotificationPtr.Pin()->ExpireAndFadeout();
	}
	
	FNotificationInfo Info( NSLOCTEXT("DistanceFieldBuild", "DistanceFieldBuildInProgress", "Building Mesh Distance Fields") );
	Info.bFireAndForget = false;
	
	// Setting fade out and expire time to 0 as the expire message is currently very obnoxious
	Info.FadeOutDuration = 0.0f;
	Info.ExpireDuration = 0.0f;

	DistanceFieldNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);

	if (DistanceFieldNotificationPtr.IsValid())
	{
		DistanceFieldNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FDistanceFieldBuildNotificationImpl::DistanceFieldBuildFinished()
{
	// Finished all requests! Notify the UI.
	TSharedPtr<SNotificationItem> NotificationItem = DistanceFieldNotificationPtr.Pin();

	if (NotificationItem.IsValid())
	{
		NotificationItem->SetText( NSLOCTEXT("DistanceFieldBuild", "DistanceFieldBuildFinished", "Finished building Distance Fields!") );
		NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		NotificationItem->ExpireAndFadeout();

		DistanceFieldNotificationPtr.Reset();
	}
}

void FDistanceFieldBuildNotificationImpl::Tick(float DeltaTime) 
{
	if (GDistanceFieldAsyncQueue)
	{
		// Trigger a new notification if we are doing an async build, and we haven't displayed the notification recently
		if (GDistanceFieldAsyncQueue->GetNumOutstandingTasks() > 0
			&& !DistanceFieldNotificationPtr.IsValid()
			&& (FPlatformTime::Seconds() - LastEnableTime) > 5)
		{
			DistanceFieldBuildStarted();
		}
		// Disable the notification when we are no longer doing an async compile
		else if (GDistanceFieldAsyncQueue->GetNumOutstandingTasks() == 0 && DistanceFieldNotificationPtr.IsValid())
		{
			DistanceFieldBuildFinished();
		}
		else if (GDistanceFieldAsyncQueue->GetNumOutstandingTasks() > 0 && DistanceFieldNotificationPtr.IsValid())
		{
			TSharedPtr<SNotificationItem> NotificationItem = DistanceFieldNotificationPtr.Pin();

			if (NotificationItem.IsValid())
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("BuildTasks"), FText::AsNumber( GDistanceFieldAsyncQueue->GetNumOutstandingTasks() ) );
				FText ProgressMessage = FText::Format(NSLOCTEXT("DistanceFieldBuild", "DistanceFieldBuildInProgressFormat", "Building Mesh Distance Fields ({BuildTasks})"), Args);

				NotificationItem->SetText( ProgressMessage );
			}
		}
	}
}

TStatId FDistanceFieldBuildNotificationImpl::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDistanceFieldBuildNotificationImpl, STATGROUP_Tickables);
}
