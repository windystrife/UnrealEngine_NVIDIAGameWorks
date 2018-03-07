// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISlateReflectorModule.h"
#include "SlateViewer.h"
#include "Widgets/Docking/SDockTab.h"


/**
 * Run the SlateViewer .
 */
int RunSlateViewer(const TCHAR* Commandline);

/**
 * Spawn the contents of the web browser tab
 */
TSharedRef<SDockTab> SpawnWebBrowserTab(const FSpawnTabArgs& Args);
