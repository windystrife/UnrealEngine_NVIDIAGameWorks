// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LocationServicesIOSImplModule.h"
#include "Classes/LocationServicesBPLibrary.h"

IMPLEMENT_MODULE(FLocationServicesIOSImplModule, LocationServicesIOSImpl)

void FLocationServicesIOSImplModule::StartupModule()
{
	ImplInstance = NewObject<ULocationServicesIOSImpl>();
	ULocationServices::SetLocationServicesImpl(ImplInstance);
}

void FLocationServicesIOSImplModule::ShutdownModule()
{
	ULocationServices::ClearLocationServicesImpl();
	
	ImplInstance = NULL;
}