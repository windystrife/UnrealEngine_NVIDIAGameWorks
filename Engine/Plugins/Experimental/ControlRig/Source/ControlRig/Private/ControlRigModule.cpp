// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigModule.h"
#include "ModuleManager.h"
#include "ControlRigObjectSpawner.h"
#include "ILevelSequenceModule.h"

void FControlRigModule::StartupModule()
{
	ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
	OnCreateMovieSceneObjectSpawnerHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FControlRigObjectSpawner::CreateObjectSpawner));

#if WITH_EDITOR
	ManipulatorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/ControlRig/M_Manip.M_Manip"));
#endif
}

void FControlRigModule::ShutdownModule()
{
	ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
	if (LevelSequenceModule)
	{
		LevelSequenceModule->UnregisterObjectSpawner(OnCreateMovieSceneObjectSpawnerHandle);
	}
}

IMPLEMENT_MODULE(FControlRigModule, ControlRig)