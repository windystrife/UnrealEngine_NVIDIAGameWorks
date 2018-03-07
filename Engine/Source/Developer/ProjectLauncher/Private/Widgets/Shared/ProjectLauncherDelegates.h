// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "ILauncherProfile.h"


/**
 * Delegate type for SProjectLauncher Profile running.
 *
 * The first parameter is the SProjectLauncher Profile that you want to run.
 */
DECLARE_DELEGATE_OneParam(FOnProfileRun, const ILauncherProfileRef&)
