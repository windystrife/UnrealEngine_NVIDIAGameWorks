// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

#include "Http.h"

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookModule.h"

/** pre-pended to all Facebook logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Facebook: ")