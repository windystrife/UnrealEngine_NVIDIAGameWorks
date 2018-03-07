// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ILauncherProfileManager.h"

class FIOSProfileWizard : public ILauncherProfileWizard
{
public:
	virtual FText GetName() const override;
	virtual FText GetDescription() const override;
	virtual void HandleCreateLauncherProfile(const ILauncherProfileManagerRef& ProfileManager) override;
};

