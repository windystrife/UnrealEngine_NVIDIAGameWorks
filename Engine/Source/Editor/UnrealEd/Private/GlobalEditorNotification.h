// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalNotification.h"
#include "Stats/Stats.h"
#include "TickableEditorObject.h"

/**
 * Class used to provide simple global editor notifications (for things like shader compilation and texture streaming) 
 */
class FGlobalEditorNotification : public FGlobalNotification, public FTickableEditorObject
{

public:
	FGlobalEditorNotification(const double InEnableDelayInSeconds = 1.0)
		: FGlobalNotification(InEnableDelayInSeconds)
	{
	}

	virtual ~FGlobalEditorNotification()
	{
	}

private:
	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
};
