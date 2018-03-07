// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCompile, Log, All);

extern bool GUHTWarningLogged;
extern bool GUHTErrorLogged;

#define UE_LOG_WARNING_UHT(Format, ...) { GUHTWarningLogged = true; UE_LOG(LogCompile, Warning, Format, ##__VA_ARGS__); }
#define UE_LOG_ERROR_UHT(Format, ...) { GUHTErrorLogged = true; UE_LOG(LogCompile, Error, Format, ##__VA_ARGS__); }
