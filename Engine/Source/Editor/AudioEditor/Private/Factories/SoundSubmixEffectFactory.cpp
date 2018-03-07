// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundSubmixEffectFactory.h"
#include "Sound/SoundSubmix.h"
#include "AudioDeviceManager.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "SoundFactoryUtility.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Modules/ModuleManager.h"
#include "Classes/Sound/AudioSettings.h"

#define LOCTEXT_NAMESPACE "AudioEditorFactories"

USoundSubmixEffectFactory::USoundSubmixEffectFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USoundEffectSubmixPreset::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
	SoundEffectSubmixPresetClass = nullptr;
}

bool USoundSubmixEffectFactory::ConfigureProperties()
{
	SoundEffectSubmixPresetClass = nullptr;

	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FAssetClassParentFilter> Filter = MakeShareable(new FAssetClassParentFilter);
	Options.ClassFilter = Filter;

	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(USoundEffectSubmixPreset::StaticClass());

	const FText TitleText = LOCTEXT("CreateSoundSubmixEffectOptions", "Pick Submix Effect Class");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, USoundEffectSubmixPreset::StaticClass());

	if (bPressedOk)
	{
		SoundEffectSubmixPresetClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* USoundSubmixEffectFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundEffectSubmixPreset* NewSoundEffectSubmixPreset = nullptr;
	if (SoundEffectSubmixPresetClass != nullptr)
	{
		NewSoundEffectSubmixPreset = NewObject<USoundEffectSubmixPreset>(InParent, SoundEffectSubmixPresetClass, InName, Flags);
	}
	return NewSoundEffectSubmixPreset;
}

bool USoundSubmixEffectFactory::CanCreateNew() const
{
	return GetDefault<UAudioSettings>()->IsAudioMixerEnabled();
}


#undef LOCTEXT_NAMESPACE