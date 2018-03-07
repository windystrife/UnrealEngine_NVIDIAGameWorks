// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GlobalEditorNotification.h"
#include "LandscapeProxy.h"
#include "Widgets/Notifications/SNotificationList.h"

/** Notification class for grassmmap rendering. */
class FLandscapeTextureBakingNotificationImpl : public FGlobalEditorNotification
{
protected:
	/** FGlobalEditorNotification interface */
	virtual bool ShouldShowNotification(const bool bIsNotificationAlreadyActive) const override;
	virtual void SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const override;
};

/** Global notification object. */
FLandscapeTextureBakingNotificationImpl GLandscapeTextureBakingNotification;

bool FLandscapeTextureBakingNotificationImpl::ShouldShowNotification(const bool bIsNotificationAlreadyActive) const
{
	return ALandscapeProxy::TotalComponentsNeedingTextureBaking > 0;
}

void FLandscapeTextureBakingNotificationImpl::SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const
{
	if (ALandscapeProxy::TotalComponentsNeedingTextureBaking > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("OutstandingTextures"), FText::AsNumber(ALandscapeProxy::TotalComponentsNeedingTextureBaking));
		const FText ProgressMessage = FText::Format(NSLOCTEXT("TextureBaking", "TextureBakingFormat", "Baking Landscape Textures ({OutstandingTextures})"), Args);
		InNotificationItem->SetText(ProgressMessage);
	}
}
