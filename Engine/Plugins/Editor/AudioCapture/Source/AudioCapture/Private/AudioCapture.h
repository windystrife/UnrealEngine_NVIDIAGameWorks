// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

// For some reason this isn't getting set
#ifndef __WINDOWS_DS__
#define __WINDOWS_DS__
#endif // __WINDOWS_DS__

// And for some reason this is still getting set...
#ifdef __WINDOWS_WASAPI__
#undef __WINDOWS_WASAPI__
#endif