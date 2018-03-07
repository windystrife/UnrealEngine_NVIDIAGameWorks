// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheModule.h"
#if WITH_EDITOR
#include "GeometryCacheEdModule.h"
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FGeometryCacheModule, GeometryCache)

void FGeometryCacheModule::StartupModule()
{
#if WITH_EDITOR
	FGeometryCacheEdModule& Module = FModuleManager::LoadModuleChecked<FGeometryCacheEdModule>(TEXT("GeometryCacheEd"));
#endif
}

void FGeometryCacheModule::ShutdownModule()
{
}
