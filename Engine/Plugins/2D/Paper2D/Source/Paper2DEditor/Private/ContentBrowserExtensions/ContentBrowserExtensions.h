// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Integrate Paper2D actions associated with existing engine types (e.g., Texture2D) into the content browser
class FPaperContentBrowserExtensions
{
public:
	static void InstallHooks();
	static void RemoveHooks();
};
