// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LocationServicesAndroidImplModule.h"
#include "LocationServicesAndroidImpl.h"
#include "Classes/LocationServicesBPLibrary.h"

IMPLEMENT_MODULE(FLocationServicesAndroidImplModule, LocationServicesAndroidImpl)

void FLocationServicesAndroidImplModule::StartupModule()
{
	ImplInstance = NewObject<ULocationServicesAndroidImpl>();
	ULocationServices::SetLocationServicesImpl(ImplInstance);
}

void FLocationServicesAndroidImplModule::ShutdownModule()
{
	ULocationServices::ClearLocationServicesImpl();
	ImplInstance = NULL;
}