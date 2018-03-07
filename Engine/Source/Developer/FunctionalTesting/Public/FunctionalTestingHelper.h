// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AutomationCommon.h"

/* Wait for all running functional tests to finish */
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(FUNCTIONALTESTING_API, FWaitForFTestsToFinish);

/* Trigger all functional tests on map */
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(FUNCTIONALTESTING_API, FTriggerFTests);

/* Trigger specific functional test on map */
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FUNCTIONALTESTING_API, FTriggerFTest, FString, TestName);

/* Start all functional tests on map and wait for them to finish */
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(FUNCTIONALTESTING_API, FStartFTestsOnMap);

/* Start specific functional test on map and wait for it to finish */
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FUNCTIONALTESTING_API, FStartFTestOnMap, FString, TestName);
