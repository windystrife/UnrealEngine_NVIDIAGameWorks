// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GlobalEditorNotification.h"
#include "LandscapeProxy.h"
#include "Widgets/Notifications/SNotificationList.h"

/** Notification class for grassmmap rendering. */
class FGrassRenderingNotificationImpl : public FGlobalEditorNotification
{
protected:
	/** FGlobalEditorNotification interface */
	virtual bool ShouldShowNotification(const bool bIsNotificationAlreadyActive) const override;
	virtual void SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const override;
};

/** Global notification object. */
FGrassRenderingNotificationImpl GGrassRenderingNotification;

bool FGrassRenderingNotificationImpl::ShouldShowNotification(const bool bIsNotificationAlreadyActive) const
{
	return ALandscapeProxy::TotalComponentsNeedingGrassMapRender > 0;
}

void FGrassRenderingNotificationImpl::SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const
{
	if (ALandscapeProxy::TotalComponentsNeedingGrassMapRender > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("OutstandingGrassMaps"), FText::AsNumber(ALandscapeProxy::TotalComponentsNeedingGrassMapRender));
		const FText ProgressMessage = FText::Format(NSLOCTEXT("GrassMapRender", "GrassMapRenderFormat", "Building Grass Maps ({OutstandingGrassMaps})"), Args);
		InNotificationItem->SetText(ProgressMessage);
	}
}
