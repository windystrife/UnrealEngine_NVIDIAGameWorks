// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetNotifications.h"
#include "Animation/Skeleton.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AssetNotifications"

void FAssetNotifications::SkeletonNeedsToBeSaved(const USkeleton* Skeleton)
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("SkeletonName"), FText::FromString( Skeleton->GetName() ) );
	FNotificationInfo Info( FText::Format( LOCTEXT("SkeletonNeedsToBeSaved", "Skeleton {SkeletonName} needs to be saved"), Args ) );
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_None );
	}
}

#undef LOCTEXT_NAMESPACE
