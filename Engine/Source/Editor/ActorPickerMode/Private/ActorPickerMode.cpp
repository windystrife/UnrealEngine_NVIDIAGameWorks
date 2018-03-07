// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorPickerMode.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EditorModeActorPicker.h"

IMPLEMENT_MODULE( FActorPickerModeModule, ActorPickerMode );

void FActorPickerModeModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FEdModeActorPicker>(FBuiltinEditorModes::EM_ActorPicker);
}

void FActorPickerModeModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_ActorPicker);
}

void FActorPickerModeModule::BeginActorPickingMode(FOnGetAllowedClasses InOnGetAllowedClasses, FOnShouldFilterActor InOnShouldFilterActor, FOnActorSelected InOnActorSelected)
{
	// Activate the mode
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_ActorPicker);

	// Set the required delegates
	FEdModeActorPicker* Mode = GLevelEditorModeTools().GetActiveModeTyped<FEdModeActorPicker>(FBuiltinEditorModes::EM_ActorPicker);
	if (ensure(Mode))
	{
		Mode->OnActorSelected = InOnActorSelected;
		Mode->OnGetAllowedClasses = InOnGetAllowedClasses;
		Mode->OnShouldFilterActor = InOnShouldFilterActor;
	}
}

void FActorPickerModeModule::EndActorPickingMode()
{
	GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_ActorPicker);
}

bool FActorPickerModeModule::IsInActorPickingMode() const
{
	return GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_ActorPicker);
}
