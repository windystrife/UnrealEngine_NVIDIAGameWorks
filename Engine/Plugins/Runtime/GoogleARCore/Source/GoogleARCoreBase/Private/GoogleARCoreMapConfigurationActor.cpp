// Copyright 2017 Google Inc.

#include "GoogleARCoreMapConfigurationActor.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreDevice.h"

void AGoogleARCoreMapConfigurationActor::BeginPlay()
{
	Super::BeginPlay();

	FGoogleARCoreDevice::GetInstance()->UpdateTangoConfiguration(GoogleARCoreSessionConfiguration);
}

void AGoogleARCoreMapConfigurationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	FGoogleARCoreDevice::GetInstance()->ResetTangoConfiguration();
}
