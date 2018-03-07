// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "NotificationManager.h"
#include "TickableEditorObject.h"

class FRenderDocPluginNotification : public FTickableEditorObject
{
public:
	static FRenderDocPluginNotification& Get()
	{
		static FRenderDocPluginNotification Instance;
		return Instance;
	}

	void ShowNotification(const FText& Message);
	void HideNotification();

protected:
	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const
	{
		return true;
	}
	virtual TStatId GetStatId() const override;

private:
	FRenderDocPluginNotification();
	FRenderDocPluginNotification(FRenderDocPluginNotification const& Notification);
	void operator=(FRenderDocPluginNotification const&);

	/** The source code symbol query in progress message */
	TWeakPtr<SNotificationItem> RenderDocNotificationPtr;
	double LastEnableTime;
};

#endif
